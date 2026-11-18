/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat Release 3 (step 7, the loader; PROJECT_FORMAT_R3.md 3, 4, 9): the pool and the project slots through the format core. Included by project.c after
 * oc_pj4_proj.c (OC_PJ4 only). Replaces, behind the same names, what oc_pool.c kept in sixteen shared chunks and oc_project.c in one small record a slot.
 *
 * The owner's decision (November 15, 2026, option 2): a phrase a keep writes is held in RAM until the project is saved, like every other edit; the autosave (stopped,
 * untouched a while) or a SAVE writes it, and a power cut before loses what any edit loses. A project with no slot yet holds them in RAM the same way.
 *   - The runtime pool is the loaded project's phrases: ids 0..63 (phrase id = pool id + 1, so no chain, song row or scene is renumbered; 255 comes with the editor).
 *     They are the slot's committed chunks (pj4 chunks of the slot proj_cur names) overlaid by pjp.has[]: the kept ones, in pjp.buf (RAM, .pool). The overlay
 *     ends (is written, or dropped) when proj_cur changes (pjp_follow) or a save commits.
 *   - ocp_open_pattern reads through pjp_open, ocp_chunk_write / ocp_copy write the overlay (pjp_put_chunk, pjp_copy), ocp_scan rebuilds the tables (pjp_scan). A
 *     slot read (ocj_read) sees committed phrases only: what the slot IS, never the overlay.
 *   - ocj_write_pj4 saves the music into a slot: the phrases of the loaded project (committed and kept, none dropped: a project owns its phrases) or, for a record that
 *     is not the music's (a backup restored), the phrases of its banks; then pj4_save: changed chunks under the other generation, the record last.
 *   - pjp_boot: the conversion of every slot that still has an old record (pj4_boot with the sections hook), then the tables.
 * Nothing is shared between slots any more: ocj_refs is 0 for every phrase.
 *
 * Revisions:
 *   Release 3 step 7 (r3-format2, step 2, the loader): first version.
 */
#define PJP_BUF 16384u                         /* RAM for kept, unsaved phrases (about 90 of the owner's average); past it a keep says PHRASES FULL until the autosave writes them */
static struct {
    uint8_t home;                              /* proj_cur as the overlay and the tables were last made for it */
    uint8_t has[OCP_N];                        /* 1: phrase id is kept in RAM (length 0: emptied) */
    uint16_t len[OCP_N], off[OCP_N];
    uint32_t used;                             /* bytes of buf in use, with the holes of replaced phrases */
    uint8_t buf[PJP_BUF];
} pjp __attribute__((section(".pool")));
#if OC_BORROW
static pj4_ws_t *pjp_wsp;                      /* (Round 30 item 1) the format core's work area: borrowed for a save or an erase (res_borrow.c), at boot for the conversion; 0 otherwise */
#define pjp_ws (*pjp_wsp)
#else
static pj4_ws_t pjp_ws __attribute__((section(".pool")));
#endif
static pj4_dir_t pjp_dir __attribute__((section(".pool")));
static uint8_t pjp_dir_slot = 0xFFu;           /* the slot pjp_dir is of (0xFF none), and whether it read */
static uint8_t pjp_dir_ok;
static uint8_t pjp_rslot = 0xFFu;              /* the slot a read is for (0xFF: the loaded project's) */
static uint8_t pjp_nopend;                     /* a read of a slot as it is stored: the kept phrases are not seen */
/* One scratch area for every operation (a save, a slot read, the boot's conversion: never two at once, the main loop's, stopped): the record's sections at the front, then
 * the project part as the record keeps it (made apart so the staging keeps the music's) / a phrase generated whole / a section being read, then the frozen table. The
 * boot's conversion takes it whole (PJ4_WORK). */
#define PJP_SEC_AT 0u
#define PJP_PK_AT PJ4_RECMAX
#define PJP_FRZ_AT (PJP_PK_AT + PROJ_STORE_SIZE)
#define PJP_FRZ_LEN 3072u
_Static_assert(PJP_FRZ_AT + PJP_FRZ_LEN <= PJ4_WORK, "the scratch holds the sections, the project part and the frozen table");
_Static_assert(PJ4_PHMAX + 8u <= PROJ_STORE_SIZE && PJ4_SECMAX <= PROJ_STORE_SIZE, "a phrase and a section fit where the project part is made");
#if OC_BORROW
/* (Round 30 item 1) No resident scratch but the reads' section buffer: a save borrows three pieces from the audio arena while the transport is stopped (res_borrow.c),
 * the sections (PJ4_RECMAX), the project part (PROJ_STORE_SIZE: then the frozen table once the sections are built, then a phrase generated whole during the write) and
 * the core's work area; the boot's conversion borrows PJ4_WORK and the work area before the audio starts. A read (ocj_read: a load, the slot list, a backup) needs only
 * one section at a time: the 4 KB below, so a load is as before, playing or not */
static uint8_t *pjp_secp, *pjp_pkp, *pjp_bootw;
static uint8_t pjp_rbuf[PJ4_SECMAX] __attribute__((section(".pool"), aligned(4)));
#define pjp_sec pjp_secp
#define pjp_pk (*(project_store_t *)(void *)pjp_pkp)
#define pjp_gbuf pjp_pkp
#define pjp_tmp pjp_rbuf
#define pjp_frzr pjp_pkp                      /* (after pj4p_build: the project part has been coded into the sections; pj4p_build writes only into its out) */
#define pjp_work pjp_bootw
_Static_assert(PJP_FRZ_LEN <= PROJ_STORE_SIZE, "the frozen table fits where the project part was");
#else
static uint8_t pjp_work[PJ4_WORK] __attribute__((section(".pool"), aligned(4)));
#define pjp_sec (pjp_work + PJP_SEC_AT)
#define pjp_pk (*(project_store_t *)(void *)(pjp_work + PJP_PK_AT))
#define pjp_gbuf (pjp_work + PJP_PK_AT)
#define pjp_tmp (pjp_work + PJP_PK_AT)
#define pjp_frzr (pjp_work + PJP_FRZ_AT)
#endif
static uint32_t pjp_rs = 0x9E3779B9u;
static uint8_t pjp_freeze;                     /* a load that rebuilds the same music (tracks.c): proj_cur may pass through none, the kept phrases stay */

static uint32_t pjp_home_slot(void) { return proj_cur < 4u ? proj_cur : 0xFFu; }
static uint32_t pjp_slot_r(void) { return pjp_rslot < 4u ? pjp_rslot : pjp_home_slot(); }
static int pjp_dirty(void) { for (uint32_t i = 0; i < OCP_N; i++) if (pjp.has[i]) return 1; return 0; }
static void pjp_drop(void) { memset(pjp.has, 0, sizeof pjp.has); memset(pjp.len, 0, sizeof pjp.len); memset(pjp.off, 0, sizeof pjp.off); pjp.used = 0; }

/* the directory of slot (cached: a slot's record is only rewritten by this file) */
static int pjp_dir_get(uint32_t slot)
{
    if (pjp_dir_slot == slot) return pjp_dir_ok;
    pjp_dir_ok = pj4_dir_load(slot, &pjp_dir) == 0;
    pjp_dir_slot = (uint8_t)slot;
    return pjp_dir_ok;
}
static void pjp_dir_forget(void) { pjp_dir_slot = 0xFFu; }
/* phrase id1 (1..) of slot as stored: its chunk record and where it is. 1 found, 0 none, -1 unreadable */
static int pjp_locate(uint32_t slot, uint32_t id1, ocs_rec_t *r, uint32_t *off, uint32_t *len)
{
    uint32_t n, pos = 0;
    if (!pjp_dir_get(slot) || id1 < 1u || id1 > PJ4_NPH) return 0;
    n = pjp_dir.chunk[id1];
    if (n == PJ4_NONE) return 0;
    if (ocs_open(OCS_POOL, pj4_cid(slot, (uint32_t)pj4_gen(&pjp_dir, n), n), r)) return -1;
    while (pos + 3u <= r->total) {
        uint8_t e[3];
        uint32_t l;
        if (ocs_at(r, pos, e, 3)) return -1;
        l = pj4_g16(e + 1);
        if (l > r->total - pos - 3u) return -1;
        if (e[0] == id1) { *off = pos + 3u; *len = l; return 1; }
        pos += 3u + l;
    }
    return -1;
}

/* ------------------------------------------------------------ the pool --- */
static int pjp_open(uint32_t id, ocs_rec_t *r, ocp_rd_t *rd)
{
    uint32_t off, len, slot;
    int rc;
    memset(rd, 0, sizeof *rd);
    if (id >= OCP_N) return -1;
    if (!pjp_nopend && pjp_rslot >= 4u && pjp.has[id]) {            /* (the loaded project's: what was kept) */
        if (!pjp.len[id]) return 0;
        rd->mem = pjp.buf + pjp.off[id]; rd->off = 0; rd->end = pjp.len[id];
        return 1;
    }
    slot = pjp_slot_r();
    if (slot > 3u) return 0;
    if ((rc = pjp_locate(slot, id + 1u, r, &off, &len)) <= 0) return rc;
    if (!len) return 0;
    rd->r = r; rd->off = off; rd->end = off + len;
    return 1;
}
static void pjp_compact(void)
{
    uint8_t order[OCP_N];
    uint32_t n = 0, i, j, at = 0;
    for (i = 0; i < OCP_N; i++) if (pjp.has[i] && pjp.len[i]) order[n++] = (uint8_t)i;
    for (i = 1; i < n; i++) {                                         /* by where they are now */
        uint8_t v = order[i];
        for (j = i; j > 0 && pjp.off[order[j - 1u]] > pjp.off[v]; j--) order[j] = order[j - 1u];
        order[j] = v;
    }
    for (i = 0; i < n; i++) {
        uint32_t id = order[i];
        for (uint32_t m = 0; m < pjp.len[id]; m++) pjp.buf[at + m] = pjp.buf[pjp.off[id] + m];   /* (down: forward is safe; the firmware has no memmove) */
        pjp.off[id] = (uint16_t)at; at += pjp.len[id];
    }
    pjp.used = at;
}
/* room for n more bytes (id's own bytes, held now, do not count: it is being replaced). 0, else 2 */
static int pjp_room(uint32_t id, uint32_t n)
{
    if (pjp.has[id]) { pjp.has[id] = 0; pjp.len[id] = 0; }            /* (its old bytes become a hole) */
    if (pjp.used + n <= PJP_BUF) return 0;
    pjp_compact();
    return pjp.used + n <= PJP_BUF ? 0 : 2;
}
static uint32_t pjp_len_of(const uint8_t *b, uint32_t n)
{
    return n >= OCP_HEAD && (b[0] == OCP_VER || b[0] == 1u || b[0] == OCP_VER_CTL) ? (uint32_t)clamp((int16_t)ocp_get16(b + 4), 1, NSTEP) : 0u;
}
static uint32_t pjp_hash_of(const uint8_t *b, uint32_t n)
{
    return n ? (ocs_crc32(b, n) | 1u) : 0u;
}
static void pjp_set(uint32_t id, uint32_t off, uint32_t n)
{
    pjp.has[id] = 1; pjp.off[id] = (uint16_t)off; pjp.len[id] = (uint16_t)n;
    ocp.len[id] = (uint8_t)pjp_len_of(pjp.buf + off, n);
    ocp.hash[id] = pjp_hash_of(pjp.buf + off, n);
    ocp.gen++;
}
/* phrase id kept as g gives it (length 0: emptied). 0; 2 no room */
static int pjp_put(uint32_t id, ocp_gen_t *g)
{
    uint32_t n = g->len, at;
    if (id >= OCP_N) return 1;
    if (!n) { if (pjp_room(id, 0)) return 2; pjp.has[id] = 1; pjp.len[id] = 0; pjp.off[id] = 0; ocp.len[id] = 0; ocp.hash[id] = 0; ocp.gen++; return 0; }
    if (n > PJ4_PHMAX || pjp_room(id, n)) return 2;
    at = pjp.used;
    ocp_gen_rewind(g);
    if (ocp_gen_get(&g->src, pjp.buf + at, n)) { ocp_gen_rewind(g); return 2; }
    ocp_gen_rewind(g);
    pjp.used += n;
    pjp_set(id, at, n);
    return 0;
}
static int pjp_put_chunk(uint32_t chunk, ocp_gen_t *const *gen)
{
    for (uint32_t j = 0; j < OCP_PER; j++)
        if (gen[j]) { int rc = pjp_put(chunk * OCP_PER + j, gen[j]); if (rc) return rc; }
    return 0;
}
/* phrase src's bytes into dst's place (RAM). 0; 1 src empty or unreadable; 2 no room */
static int pjp_copy(uint32_t src, uint32_t dst)
{
    static ocp_rd_t rd __attribute__((section(".pool")));
    ocs_rec_t r;
    uint32_t n, at;
    int rc;
    if (src >= OCP_N || dst >= OCP_N || src == dst) return 1;
    if ((rc = pjp_open(src, &r, &rd)) != 1) return 1;
    n = rd.end - rd.off;
    if (n > PJ4_PHMAX) return 1;
    if (pjp_room(dst, n)) return 2;                                     /* (room first: a compaction moves the bytes src may be read from, so it is opened again) */
    if (pjp_open(src, &r, &rd) != 1) return 1;
    at = pjp.used;
    if (ocp_rd(&rd, pjp.buf + at, n)) return 1;
    pjp.used += n;
    pjp_set(dst, at, n);
    return 0;
}
static void pjp_scan(void)
{
    uint32_t slot, id;
    memset(&ocp, 0, sizeof ocp);
    slot = pjp_slot_r();
    if (slot < 4u && pjp_dir_get(slot))
        for (id = 0; id < OCP_N; id++) {
            ocs_rec_t r;
            uint32_t off, len;
            if (pjp_locate(slot, id + 1u, &r, &off, &len) != 1 || !len) continue;
            ocp.len[id] = (uint8_t)ocp_len_of(&r, off, len);
            ocp.hash[id] = pjp_dir.crc[id + 1u] | 1u;
        }
    for (id = 0; id < OCP_N && !pjp_nopend && pjp_rslot >= 4u; id++)
        if (pjp.has[id]) {
            ocp.len[id] = (uint8_t)(pjp.len[id] ? pjp_len_of(pjp.buf + pjp.off[id], pjp.len[id]) : 0u);
            ocp.hash[id] = pjp.len[id] ? pjp_hash_of(pjp.buf + pjp.off[id], pjp.len[id]) : 0u;
        }
    ocp.gen++;
}
/* the store was read again (a boot, a restore): what was kept in RAM is gone, the tables are made from the store */
static void pjp_reset(void) { pjp_drop(); pjp_dir_forget(); pjp.home = (uint8_t)proj_cur; pjp_scan(); }
/* proj_cur changed (a load, a save to another slot, an erase): the overlay was the old music's, the tables are made again for the new one */
static void pjp_follow(void)
{
    if (pjp_freeze || pjp.home == proj_cur) return;
    pjp.home = proj_cur;
    pjp_drop();
    pjp_scan();
}
/* every committed phrase of the loaded project held in RAM too: the slot is about to go (an erase), the music in RAM keeps what it names. 0, 2 no room */
static int pjp_hold_all(void)
{
    uint32_t id, slot = pjp_home_slot();
    if (slot > 3u) return 0;
    for (id = 0; id < OCP_N; id++) {
        ocs_rec_t r;
        uint32_t off, len;
        if (pjp.has[id] || pjp_locate(slot, id + 1u, &r, &off, &len) != 1 || !len) continue;
        if (pjp_room(id, len)) return 2;
        {
            ocp_rd_t rd;
            memset(&rd, 0, sizeof rd);
            rd.r = &r; rd.off = off; rd.end = off + len;
            if (ocp_rd(&rd, pjp.buf + pjp.used, len)) return 2;
        }
        pjp_set(id, pjp.used, len);
        pjp.used += len;
    }
    return 0;
}

/* ------------------------------------------------------------ the save --- */
typedef struct {
    int xs;                                     /* the phrases are the loaded project's (committed or kept), else its banks' (a record that is not the music's) */
    const project_t *q; const uint8_t *raw;
    uint8_t bank[PJ4_NPH + 1u];                 /* !xs: phrase id -> bank (k * NPAT + b + 1) */
    uint32_t cached;                            /* !xs: the phrase pjp_gbuf holds */
    uint32_t home;                              /* xs: the slot the committed phrases are in */
} pjp_save_t;
static int pjp_get(void *ctx, uint32_t id, uint8_t *dst, uint32_t off, uint32_t n)
{
    pjp_save_t *c = ctx;
    if (c->xs) {
        ocs_rec_t r;
        uint32_t o, l;
        if (id > OCP_N) return -1;
        if (pjp.has[id - 1u]) {
            if (off + n > pjp.len[id - 1u]) return -1;
            memcpy(dst, pjp.buf + pjp.off[id - 1u] + off, n);
            return 0;
        }
        if (c->home > 3u || pjp_locate(c->home, id, &r, &o, &l) != 1 || off + n > l) return -1;
        return ocs_at(&r, o + off, dst, n);
    }
    if (c->cached != id) {
        static ocp_gen_t g __attribute__((section(".pool")));
        uint32_t bk = c->bank[id] - 1u;
        ocj_gen_fbkk(&g, c->q, c->raw, bk / NPAT, bk % NPAT, 0);
        if (!ocp_gen_count(&g) || g.len > PJ4_PHMAX || ocp_gen_get(&g.src, pjp_gbuf, g.len)) return -1;
        c->cached = id;
    }
    memcpy(dst, pjp_gbuf + off, n);
    return 0;
}
static uint32_t pjp_next(void) { pjp_rs ^= pjp_rs << 13; pjp_rs ^= pjp_rs >> 17; pjp_rs ^= pjp_rs << 5; return pjp_rs; }

/* FBKK raw (BANK_STORE_SIZE; 0: the slot emptied) into slot as a project of the revision's format. 0 done; 2 not (PHRASES FULL, PROJECT FULL, a flash error: the slot as it
 * was). raw is used, and left as the slot reads back (ocj_read) when it is done */
#if OC_BORROW
static int ocj_write_pj4_in(uint32_t slot, uint8_t *raw, uint32_t len);
static uint8_t pjp_bor_rc;                     /* (Round 30) the last save's borrow: RES_BOR_* (the tests, AUDIO_STATS) */
/* (Round 30 item 1) the save's scratch borrowed from the audio arena for the save only. 3: not now, the transport runs (the caller waits for the stop: UNSAVED);
 * 2: not saved (the slot as it was), as before; 0 done */
static int ocj_write_pj4(uint32_t slot, uint8_t *raw, uint32_t len)
{
    uint32_t sz[3];
    uint8_t *p[3];
    int rc;
    sz[0] = len ? PJ4_RECMAX : 0u;             /* (an erase: only the work area) */
    sz[1] = len ? PROJ_STORE_SIZE : 0u;
    sz[2] = (uint32_t)sizeof(pj4_ws_t);
    rc = res_borrow(sz, 3u, p);
    pjp_bor_rc = (uint8_t)rc;
    if (rc == RES_BOR_PLAYING) return 3;
    if (rc) return 2;                          /* (the caller says NO MEMORY TO SAVE: pjp_bor_rc) */
    pjp_secp = p[0]; pjp_pkp = p[1]; pjp_wsp = (pj4_ws_t *)(void *)p[2];
    rc = ocj_write_pj4_in(slot, raw, len);
    pjp_secp = pjp_pkp = 0; pjp_wsp = 0;
    res_borrow_end();
    return rc;
}
static int ocj_write_pj4_in(uint32_t slot, uint8_t *raw, uint32_t len)
#else
static int ocj_write_pj4(uint32_t slot, uint8_t *raw, uint32_t len)
#endif
{
    static uint8_t want[OCP_N] __attribute__((section(".pool")));
    static pjp_save_t sv __attribute__((section(".pool")));
    static uint8_t gen_of[PJ4_NPH + 1u] __attribute__((section(".pool")));
    uint8_t npid[NTRK][NPAT];
    uint32_t k, b, id, hash[NTRK][NPAT], nph = 0, nsec = 0, over = 0, home, cmap[OCP_N + 1u];
    project_t *q = &proj_scratch;
    pj4_phr_t *ph;
    pj4_head_t hd, old;
    pj4_sec_t secs[PJ4_NSEC];
    int xs, rc;
    slot &= 3u;
    home = pjp_home_slot();
    if (!len) {                                /* emptied: the music in RAM keeps the phrases it names, held until it is saved somewhere */
        if (slot == home && pjp_hold_all()) return 2;
        pj4_w = &pjp_ws;
        rc = pj4_slot_clear(&pjp_ws, slot);
        pjp_dir_forget();
        return rc ? 2 : 0;
    }
    xs = proj_x_tag == proj_hash(raw, BANK_STORE_SIZE);
    if (len != BANK_STORE_SIZE || !bank_valid(raw, len)) return 2;
    /* 1. each bank: what it holds, and the phrase it is */
    memset(npid, 0, sizeof npid);
    memset(gen_of, 0, sizeof gen_of);
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++) {
            hash[k][b] = 0;
            if (ocj_gen_fbkk(&ocj_gen[0], q, raw, k, b, xs)) hash[k][b] = ocp_gen_hash(&ocj_gen[0]);
        }
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++) {
            if (!hash[k][b]) continue;
            if (xs && proj_pid[k][b] && proj_pid[k][b] <= OCP_N) { npid[k][b] = proj_pid[k][b]; continue; }   /* (the phrase the bank is, as named) */
            for (id = 1; id <= OCP_N; id++) {                          /* a free one: no phrase of the project, no bank here has it */
                uint32_t taken = xs && (ocp.len[id - 1u] || ocp.hash[id - 1u] || ocj_named_now(id - 1u, 1));
                for (uint32_t k2 = 0; k2 < NTRK && !taken; k2++)
                    for (uint32_t b2 = 0; b2 < NPAT; b2++) taken |= npid[k2][b2] == id;
                if (!taken) break;
            }
            if (id > OCP_N) { ui_message("PHRASES FULL"); return 2; }
            npid[k][b] = (uint8_t)id;
        }
    /* 2. the phrases that changed: kept (RAM) under the loaded project, as a keep does */
    if (xs) {
        memset(want, 0, sizeof want);
        for (k = 0; k < NTRK; k++)
            for (b = 0; b < NPAT; b++)
                if ((id = npid[k][b]) != 0u && ocp.hash[id - 1u] != hash[k][b] && proj_pid[k][b] != id)
                    want[id - 1u] = (uint8_t)(k * NPAT + b + 1u);
        if (ocj_put_chunks(want, q, raw, xs)) { ui_message("PHRASES FULL"); return 2; }
    } else {
        for (k = 0; k < NTRK; k++)
            for (b = 0; b < NPAT; b++)
                if (npid[k][b] && !gen_of[npid[k][b]]) gen_of[npid[k][b]] = (uint8_t)(k * NPAT + b + 1u);
    }
    /* 3. the record's head and lists, the project part without its steps, timed notes and motion */
    memset(&ocj_h, 0, sizeof ocj_h);
    ocj_h.magic = OCJ_MAGIC;
    ocj_h.lsize = (uint32_t)sizeof ocj_l;
    memcpy(ocj_h.pid, npid, sizeof npid);
    memcpy(ocj_h.active, raw + BANK_ACTIVE_OFF, NTRK);
    memcpy(ocj_h.chain, raw + BANK_CHAIN_OFF, sizeof ocj_h.chain);
    memcpy(ocj_h.name, q->name, PROJ_NAME_LEN);
    memcpy(ocj_h.fn, raw + BANK_FN_OFF, OCJ_FN);
    if (xs) ocj_l = proj_lists; else ocl_lists_init(&ocj_l);
    for (k = 0; k < NTRK; k++)
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&q->t[k].step[i]);
    memset(q->recording, 0, sizeof q->recording);
    q->motion.count = 0;
    memset(q->motion.event, 0, sizeof q->motion.event);
    if (!proj_pack(&pjp_pk, q)) return 2;
    if (!xs && !proj_import(q, raw + 8u, PROJ_STORE_SIZE)) return 2;       /* (the banks' phrases are generated from the music in q: it is back) */
    /* 4. the phrases of the project: ascending ids, each with its length and CRC */
    pj4_w = &pjp_ws;
    ph = pjp_ws.ph;
    memset(cmap, 0, sizeof cmap);
    sv.xs = xs; sv.q = q; sv.raw = raw; sv.cached = 0; sv.home = home;
    memcpy(sv.bank, gen_of, sizeof sv.bank);
    for (id = 1; id <= OCP_N; id++) {
        uint32_t l = 0, c = 0;
        if (xs) {
            if (pjp.has[id - 1u]) { l = pjp.len[id - 1u]; c = l ? ocs_crc32(pjp.buf + pjp.off[id - 1u], l) : 0u; }
            else if (home < 4u) {
                ocs_rec_t r;
                uint32_t o;
                if (pjp_locate(home, id, &r, &o, &l) == 1) c = pjp_dir.crc[id]; else l = 0;
            }
        } else if (gen_of[id]) {
            uint32_t bk = gen_of[id] - 1u;
            l = ocj_gen_fbkk(&ocj_gen[0], q, raw, bk / NPAT, bk % NPAT, 0);
            if (l) {                                                  /* the CRC of the bytes as saved (ocs_crc32), not the pool's hash */
                uint32_t crc = 0xFFFFFFFFu, at = 0;
                ocp_gen_rewind(&ocj_gen[0]);
                uint8_t t[64];
                while (at < l) { uint32_t n = l - at < sizeof t ? l - at : (uint32_t)sizeof t; if (ocp_gen_get(&ocj_gen[0].src, t, n)) return 2; crc = ocs_crc_add(crc, t, n); at += n; }
                c = ~crc;
            }
        }
        if (!l) continue;
        ph[nph].id = (uint8_t)id; ph[nph].len = (uint16_t)l; ph[nph].crc = c; cmap[id] = c; nph++;
    }
    /* 5. the sections, then the save: the changed chunks under the other generation, the record last */
    memset(&hd, 0, sizeof hd);
    if (pj4_head_load(slot, &old) == 0) hd = old;
    else { hd.seed = pjp_next() ^ proj_hash(raw, 64u); hd.uid = pjp_next(); }
    if (pj4p_build(&ocj_h, &ocj_l, pjp_pk.raw, &hd, 0, cmap, pjp_sec, PJ4_RECMAX, secs, &nsec)) { ui_message("PROJECT FULL"); return 2; }
    {   /* the frozen phrases' table of the project is carried as it was (the unit's table is not read by the loader yet) */
        int fl = pj4_sec_read(slot, PJ4_T_FRZR, pjp_frzr, PJP_FRZ_LEN);
        if (fl > 0 && nsec && secs[nsec - 1u].tag == PJ4_T_FRZR) {
            uint32_t at = secs[nsec - 1u].p - pjp_sec;
            if (at + (uint32_t)fl <= PJ4_RECMAX) { memcpy(pjp_sec + at, pjp_frzr, (uint32_t)fl); secs[nsec - 1u].len = (uint32_t)fl; }
        }
    }
    rc = pj4_save(&pjp_ws, slot, &hd, secs, nsec, ph, nph, pjp_get, &sv, &over);
    pjp_dir_forget();
    if (rc == PJ4_FULL) { ui_message("PROJECT FULL"); return 2; }
    if (rc) return 2;
    if (slot == home) { pjp_drop(); pjp_scan(); }                      /* (committed: nothing is kept in RAM any more) */
    return ocj_read(slot, raw) == (int)BANK_STORE_SIZE ? 0 : 2;
}

/* stage A of ocj_read: the slot's sections as ocj_h, ocj_l and the project part at raw + 8. 0, -1 */
static int pjp_read_a(uint32_t slot, uint8_t *raw)
{
    if (pj4p_read(slot, &ocj_h, &ocj_l, raw + 8u, pjp_tmp)) return -1;
    ocl_lists_fix(&ocj_l);
    ocj_lead = (uint32_t)sizeof ocj_l;
    return proj_import(&proj_scratch, raw + 8u, PROJ_STORE_SIZE) ? 0 : -1;
}
static void pjp_ctx_enter(uint32_t slot) { pjp_rslot = (uint8_t)slot; pjp_nopend = 1; }
static void pjp_ctx_leave(void) { pjp_rslot = 0xFFu; pjp_nopend = 0; }

/* ------------------------------------------------------------ the boot --- */
static void pjp_mk(uint32_t slot, pj4_head_t *h) { h->seed = pjp_next() ^ (slot * 0x9E3779B1u); h->uid = pjp_next(); }
static void pjp_boot(void)
{
    uint64_t names[PJ4_SLOTS];
    uint8_t status[PJ4_SLOTS];
    uint32_t over[PJ4_SLOTS];
    static pj4p_ctx_t cx;
    pj4_cvhook_t hk;
    ocj_scan_old();                                                    /* (the phrases each old record names) */
    for (uint32_t s = 0; s < PJ4_SLOTS; s++) names[s] = (uint64_t)ocj.names[s][0] | (uint64_t)ocj.names[s][1] << 32;
    pjp_rs ^= fm1_ms * 2654435761u;
    {   /* the unit's frozen table (64 entries of 32 B, the first of the record): its phrases go into the projects that have them */
        ocs_rec_t r;
        cx.frz = 0;
        if (ocs_open(OCS_FREEZE, 0, &r) == 0 && r.total >= PJ4_NFRZ * PJ4_FRZ_BYTES && !ocs_at(&r, 0, proj_wire_u.raw, PJ4_NFRZ * PJ4_FRZ_BYTES)) cx.frz = proj_wire_u.raw;   /* (the staging: nothing is loaded yet; the conversion's work area is the scratch) */
    }
    hk.split = pj4p_split; hk.ctx = &cx;
#if OC_BORROW
    {   /* (Round 30 item 1) before the audio starts: the whole arena is free but for the hot sets; the conversion's scratch borrowed from it and given back */
        uint32_t sz[2] = {PJ4_WORK, (uint32_t)sizeof(pj4_ws_t)};
        uint8_t *p[2];
        if (res_borrow_boot(sz, 2u, p) == RES_BOR_OK) {
            pjp_bootw = p[0]; pjp_wsp = (pj4_ws_t *)(void *)p[1];
            pj4_w = &pjp_ws;
            pj4_boot(&pjp_ws, names, pjp_work, pjp_mk, status, over, &hk);
            pjp_bootw = 0; pjp_wsp = 0;
            res_borrow_end();
        }   /* (none: nothing converted this boot; an old record stays as it is, OLD FORMAT, and the next boot tries again) */
    }
#else
    pj4_w = &pjp_ws;
    pj4_boot(&pjp_ws, names, pjp_work, pjp_mk, status, over, &hk);
#endif
    pjp_dir_forget(); pjp_drop();
    memset(&ocj, 0, sizeof ocj);
    pjp.home = 0xFEu;                                                  /* (not any slot: the first pjp_follow makes the tables) */
    pjp_follow();
}
