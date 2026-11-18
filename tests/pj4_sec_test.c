/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 3 (step 7, step 2): host test of a project's sections, firmware/src/oc_pj4_proj.c (build with -DOC_PJ4=1):
 *   build/host/pj4_sec_test
 * 1. the cut of the project part (pj4_rng) tiles it: every byte in exactly one range, in order.
 * 2. the run-length coding over segments: random bytes with long zero runs across segment edges, and what it refuses.
 * 3. the conversion with the hook: two real projects (all banks, motion, timed notes, two tracks OFF) of the OCJ3 store become sections (no OJ3B), and the sections put
 *    the old record back byte for byte (head, lists, project part); the tracks' stable ids are distinct and the seed's; the OFF flags are the tracks'; SCNS is version 1 with
 *    the old scenes; PERF is the eight scene keys; FRZR holds the frozen phrases the project has, with the new CRC of each, less the unit's folder.
 * 4. a power cut at every program call and every erase of that conversion: the next boot finishes it, the slot reads as the old record did.
 * Everything runs on the simulated NOR of tests/oc_project_test.c (its store hooks and fixtures).
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static pj4_ws_t WS;
static uint8_t cvwork[PJ4_WORK];
static uint8_t frz[PJ4_NFRZ * PJ4_FRZ_BYTES];
static uint8_t orig[4][PJ4_RECMAX];
static int origlen[4];
static uint64_t names[4];
static uint8_t status[4];
static uint32_t over[4];
static pj4p_ctx_t cx = {frz};
static pj4_cvhook_t hk = {pj4p_split, &cx};

static void mk(uint32_t slot, pj4_head_t *h) { h->seed = 0x1000u + slot * 7919u; h->uid = 0xA0u + slot; }
static void names_now(void)
{
    for (uint32_t s = 0; s < 4u; s++) names[s] = (uint64_t)ocj.names[s][0] | (uint64_t)ocj.names[s][1] << 32;
}

/* the old record's bytes again from the sections */
static int rebuild(uint32_t slot, uint8_t *out)
{
    static ocj_head_t h;
    static ocl_lists_t l;
    static uint8_t pack[PROJ_STORE_SIZE], tmp[PJ4_SECMAX];
    ocj_src_t z;
    if (pj4p_read(slot, &h, &l, pack, tmp)) return -1;
    ocj_l = l;
    h.zlen = ocj_rle_len(pack);
    h.len = (uint32_t)sizeof h + h.zlen;
    ocj_h = h;
    z.src.get = ocj_get; z.in = pack; z.pos = 0; z.hpos = 0; z.pending = -1;
    if (ocj_get(&z.src, out, h.len)) return -1;
    return (int)h.len;
}

static int t_tiling(void)
{
    uint32_t cur = 0, i;
    int ok = 1;
    for (i = 0; i < PJ4_NRNG; i++) { ok &= pj4_rng[i].off == cur && pj4_rng[i].len > 0; cur += pj4_rng[i].len; }
    ok &= cur == PROJ_STORE_SIZE;
    return check("the cut of the project part tiles it: every byte in one range, in order", ok);
}

static uint32_t rs = 7;
static uint32_t rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int t_rle(void)
{
    static uint8_t in[3000], back[3000], coded[7000];
    int ok = 1;
    for (uint32_t it = 0; it < 400u; it++) {
        uint32_t n = 1u + rn() % 2999u, i, ns = 1u + rn() % 5u, cut[5], at = 0;
        pj4_seg_t sg[5];
        pj4_dseg_t ds[5];
        int c;
        for (i = 0; i < n; i++) in[i] = (rn() % 5u) ? 0 : (uint8_t)(1u + rn() % 255u);           /* mostly zeros: long runs, some past 255 */
        if (it % 7u == 0) memset(in, 0, n);
        for (i = 0; i < ns; i++) cut[i] = i + 1u < ns ? rn() % (n + 1u) : n;
        for (i = 0; i + 1u < ns; i++) for (uint32_t j = i + 1u; j < ns; j++) if (cut[j] < cut[i]) { uint32_t t = cut[i]; cut[i] = cut[j]; cut[j] = t; }
        for (i = 0; i < ns; i++) { sg[i].p = in + at; sg[i].n = cut[i] - at; ds[i].p = back + at; ds[i].n = cut[i] - at; at = cut[i]; }
        c = pj4_rle_enc(sg, ns, coded, sizeof coded);
        memset(back, 0xEE, sizeof back);
        ok &= c > 0 && pj4_rle_dec(coded, (uint32_t)c, ds, ns) == 0 && !memcmp(in, back, n);
        ok &= c > 1 && pj4_rle_enc(sg, ns, coded, (uint32_t)c - 1u) < 0;                           /* one byte short: refused, not overrun */
        if (c > 2) {                                                                                  /* the coding cut short is not the segments' bytes */
            pj4_dseg_t d1 = {back, n};
            ok &= pj4_rle_dec(coded, (uint32_t)c - 1u, &d1, 1) != 0 || n == 0;
        }
    }
    {
        static const uint8_t bad1[] = {5, 0}, bad2[] = {0, 0, 5}, bad3[] = {0, 200, 7};
        pj4_dseg_t d = {back, 100};
        ok &= pj4_rle_dec(bad1, 2, &d, 1) != 0 && pj4_rle_dec(bad2, 3, &d, 1) != 0 && pj4_rle_dec(bad3, 3, &d, 1) != 0;   /* a trailing 0, a 0 count, a run past the end */
        d.n = 3;
        ok &= pj4_rle_dec(bad1, 1, &d, 1) != 0;                                                      /* short: fewer bytes than the segments hold */
    }
    return check("the run-length coding over segments: 400 random cuts round trip, a short input, a zero count and a run past the end refused", ok);
}

static void make_projects(void)
{
    reset();
    fill_all(7, 3);
    trk_off_bits = 0; trk_off_set(1, 1); trk_off_set(2, 1);
    project_save_as(0, "CONV A");
    fill_all(11, 5);
    trk_off_bits = 0;
    project_save_as(1, "CONV B");
    for (uint32_t s = 0; s < 4u; s++) origlen[s] = ocs_read(OCS_PROJECT, s, orig[s], sizeof orig[s]);
    names_now();
    memset(frz, 0, sizeof frz);
    for (uint32_t id = 1; id <= 64u; id++)                                                           /* a frozen phrase for every third id, whichever project has it */
        if (id % 3u == 0) {
            uint8_t *f = frz + (id - 1u) * PJ4_FRZ_BYTES;
            f[0] = (uint8_t)id; f[1] = 0;                  /* take */
            f[2] = (uint8_t)(id % 4u); f[3] = 2;           /* trk, ver */
            f[4] = 100; f[6] = 120;                        /* lvl, bpm */
            f[8] = (uint8_t)(id * 3u); f[9] = 1;           /* frames */
            f[12] = 0x11; f[13] = 0x22; f[14] = 0x33; f[15] = 0x44;   /* the old hash: replaced */
            f[16] = 9; f[17] = 5;                          /* dir (not kept), want */
            snprintf((char *)f + 18, 14, "T%u_%u.WAV", id % 4u + 1u, id);
        }
}

static int t_convert(void)
{
    int ok = 1;
    uint8_t buf[PJ4_RECMAX];
    make_projects();
    ok &= origlen[0] > 0 && origlen[1] > 0 && origlen[2] < 0 && origlen[3] < 0;
    pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
    ok &= status[0] == 1 && status[1] == 1 && status[2] == 0 && status[3] == 0;
    for (uint32_t s = 0; s < 2u; s++) {
        int n = rebuild(s, buf);
        ok &= n == origlen[s] && n > 0 && !memcmp(buf, orig[s], (size_t)n);                        /* the old record, byte for byte */
        ok &= pj4_sec_read(s, PJ4_T_OJ3B, buf, sizeof buf) < 0;                                     /* nothing carried whole */
        ok &= !ocs_exists(OCS_PROJECT, s);                                                           /* (the old record is gone: its id is the slot) */
    }
    {   /* slot 0: the tracks' ids and flags */
        uint8_t t[PJ4_SECMAX];
        pj4_head_t hd;
        uint32_t ids[4];
        int n = pj4_sec_read(0, PJ4_T_TRKS, t, sizeof t);
        uint32_t pos = 1u, k;
        ok &= n > 0 && t[0] == 4u && pj4_head_load(0, &hd) == 0;
        for (k = 0; k < 4u && ok; k++) {
            uint32_t el = pj4_g16(t + pos);
            ids[k] = pj4_g32(t + pos + 2u);
            ok &= ids[k] == pj4_track_id(hd.seed, k) && pj4_g32(t + pos + 6u) == 0 && t[pos + 11u] == 0;
            ok &= (t[pos + 12u] & 1u) == (k == 1u || k == 2u);                                       /* OFF: tracks 2 and 3 */
            pos += 2u + el;
        }
        ok &= ids[0] != ids[1] && ids[0] != ids[2] && ids[0] != ids[3] && ids[1] != ids[2] && ids[1] != ids[3] && ids[2] != ids[3];
        ok &= pos == (uint32_t)n && !strncmp(hd.name, "CONV A", 6) && hd.next_id == 1u;
    }
    {   /* SCNS, PERF and FRZR of slot 0 */
        uint8_t t[PJ4_SECMAX], ph[PJ4_PHMAX + 8];
        int n = pj4_sec_read(0, PJ4_T_SCNS, t, sizeof t);
        uint32_t i, cnt, pos, seen = 0;
        ok &= n == (int)(2u + PJ4_SCN_BYTES + 1u) && t[0] == 1u && t[1] == OCL_NSCENE;
        ok &= pj4_sec_read(0, PJ4_T_PERF, t, sizeof t) == 256 && t[0] == 3u && t[1] == 0 && t[4 * 7] == 3u && t[4 * 7 + 1] == 7u && t[4 * 8] == 0;
        n = pj4_sec_read(0, PJ4_T_FRZR, t, sizeof t);
        ok &= n >= 1;
        cnt = t[0]; pos = 1u;
        for (i = 0; i < cnt && ok; i++) {
            uint32_t el = pj4_g16(t + pos), id = t[pos + 2u];
            int len = pj4_phrase_read(&WS, 0, id, ph, sizeof ph);
            ok &= el == 33u && id % 3u == 0 && len > 0 && pj4_g32(t + pos + 15u) == ocs_crc32(ph, (uint32_t)len);   /* the new CRC of the phrase it names */
            ok &= t[pos + 19u] == 5u && !strncmp((const char *)t + pos + 20u, "T", 1) && pj4_g16(t + pos + 3u) == id;
            seen++;
            pos += 2u + el;
        }
        ok &= pos == (uint32_t)n && seen > 0 && seen == cnt;
        {
            uint32_t have = 0;
            for (i = 1; i <= 64u; i++) if (i % 3u == 0 && pj4_phrase_read(&WS, 0, i, ph, sizeof ph) > 0) have++;
            ok &= have == seen;                                                                  /* every frozen id the project has is in FRZR, and no other */
        }
    }
    {   /* the part read back carries the OFF word and a hash that matches it (a stale hash made the project refuse to load) */
        static ocj_head_t rh; static ocl_lists_t rl; static uint8_t rp[PROJ_STORE_SIZE], rt[PJ4_SECMAX];
        uint32_t sum;
        ok &= pj4p_read(0, &rh, &rl, rp, rt) == 0;
        memcpy(&sum, rp + PROJ_STORE_SIZE - 4u, 4);
        ok &= sum == proj_hash(rp, PROJ_STORE_SIZE - 4u) && (rp[PROJ_OFF_WIRE + 1u] & 0xF0u) == 0xB0u && (rp[PROJ_OFF_WIRE] & 6u) == 6u;
    }
    return check("the conversion with the hook: sections not OJ3B, the old record back byte for byte, ids, OFF flags, SCNS 1, PERF, FRZR with the new CRCs", ok);
}

static uint32_t phr_ref[2][65];   /* a clean conversion's phrases: the CRC of each id (0: none) */
static void phr_take(uint32_t (*dst)[65])
{
    static uint8_t ph[PJ4_PHMAX + 8];
    for (uint32_t s = 0; s < 2u; s++)
        for (uint32_t i = 1; i <= 64u; i++) { int n = pj4_phrase_read(&WS, s, i, ph, sizeof ph); dst[s][i] = n > 0 ? ocs_crc32(ph, (uint32_t)n) : 0; }
}
static int phr_bad(void)       /* the phrases after a cut and the next boot are the clean run's, none lost or half written */
{
    uint32_t now[2][65], bad = 0;
    phr_take(now);
    for (uint32_t s = 0; s < 2u; s++) for (uint32_t i = 1; i <= 64u; i++) bad += now[s][i] != phr_ref[s][i];
    return (int)bad;
}

static int t_cuts(void)
{
    static uint8_t keep[0x100000];
    uint8_t buf[PJ4_RECMAX];
    uint32_t progs, cuts = 0, bad = 0, n, erases_n, mode;
    make_projects();
    memcpy(keep, nor, sizeof keep);
    erases = 0; fail_after = -1;
    pj4_boot(&WS, names, cvwork, mk, status, over, &hk);                                              /* a clean run: how many programs and erases it takes */
    erases_n = erases;
    phr_take(phr_ref);
    progs = 0;
    for (n = 0; n < 4000u; n++) {                                                                    /* programs: the first count that no longer cuts is the whole run */
        memcpy(nor, keep, sizeof nor);
        names_now();
        ocs_boot(); ocp_scan(); ocj_scan(); names_now();
        fail_after = (int)n; erase_cut = -1;
        pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
        if (fail_after > 0 || (fail_after < 0)) { progs = n; fail_after = -1; break; }              /* (not cut: it ran to the end) */
        cuts++;
        fail_after = -1;
        ocs_boot(); ocp_scan(); ocj_scan(); names_now();                                             /* the next boot */
        for (uint32_t s = 0; s < 2u; s++) {
            if (!ocs_exists(OCS_PROJECT, s) && pj4_head_load(s, &(pj4_head_t){0}) != 0) { bad++; continue; }
        }
        pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
        for (uint32_t s = 0; s < 2u; s++) {
            int r = rebuild(s, buf);
            if (r != origlen[s] || memcmp(buf, orig[s], (size_t)r)) bad++;
        }
        bad += (uint32_t)phr_bad();
    }
    for (mode = 0; mode < 1u; mode++)
        for (n = 0; n < erases_n + 2u; n++) {                                                         /* erases: the power goes right after the n-th */
            memcpy(nor, keep, sizeof nor);
            ocs_boot(); ocp_scan(); ocj_scan(); names_now();
            fail_after = -1; erase_cut = (int)n;
            pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
            fail_after = -1; erase_cut = -1;
            cuts++;
            ocs_boot(); ocp_scan(); ocj_scan(); names_now();
            pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
            for (uint32_t s = 0; s < 2u; s++) {
                int r = rebuild(s, buf);
                if (r != origlen[s] || memcmp(buf, orig[s], (size_t)r)) bad++;
            }
            bad += (uint32_t)phr_bad();
        }
    {
        char what[200];
        snprintf(what, sizeof what, "power cut at %u program and erase points of the conversion: the next boot finishes it, both slots read as the old records did (%u bad)", cuts, bad);
        return check(what, bad == 0 && cuts > 40u && progs > 0);
    }
}

int main(void)
{
    int bad = 0;
    bad |= t_tiling();
    bad |= t_rle();
    bad |= t_convert();
    bad |= t_cuts();
    printf(bad ? "pj4 sections: FAILED\n" : "pj4 sections: all passed\n");
    return bad ? 1 : 0;
}
